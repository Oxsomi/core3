/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ref_ptr.h"
#include "platforms/platform.h"
#include "types/buffer.h"
#include "types/type_id.h"
#include "types/error.h"

Error RefPtr_create(U32 objectLength, Allocator alloc, ObjectFreeFunc free, ETypeId type, RefPtr **result) {

	if(!objectLength || !free || !result)
		return Error_nullPointer(
			!result ? 3 : (!free ? 2 : 0), "RefPtr_create()::objectLength, free and result are required"
		);

	if(*result)
		return Error_invalidParameter(3, 0, "RefPtr_create()::result isn't empty, might indicate memleak");

	Buffer buf = Buffer_createNull();
	Error err = Buffer_createEmptyBytes(sizeof(RefPtr) + objectLength, alloc, &buf);

	if(err.genericError)
		return err;

	*(*result = (RefPtr*)buf.ptr) = (RefPtr) {
		.refCount = (AtomicI64) { 1 },
		.typeId = type,
		.length = objectLength,
		.alloc = alloc,
		.free = free
	};

	return Error_none();
}

Error RefPtr_createx(U32 objectLength, ObjectFreeFunc free, ETypeId type, RefPtr **result) {
	return RefPtr_create(objectLength, Platform_instance.alloc, free, type, result);
}

Bool RefPtr_inc(RefPtr *ptr) {

	if(!ptr || !ptr->free)
		return false;

	AtomicI64_inc(&ptr->refCount);
	return true;
}

Bool RefPtr_dec(RefPtr **pptr) {

	if(!pptr || !*pptr)
		return true;

	RefPtr *ptr = *pptr;

	Bool b = true;

	if(!AtomicI64_dec(&ptr->refCount)) {

		b = ptr->free(RefPtr_data(ptr, void), ptr->alloc);

		Buffer orig = Buffer_createManagedPtr(ptr, sizeof(*ptr) + ptr->length);
		b &= Buffer_free(&orig, ptr->alloc);
	}

	*pptr = NULL;
	return b;
}
