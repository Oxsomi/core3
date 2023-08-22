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

RefPtr RefPtr_create(void *ptr, Allocator alloc, ObjectFreeFunc free) {

	if(!ptr)
		return (RefPtr) { 0 };

	return (RefPtr) {
		.refCount = 1,
		.ptr = ptr,
		.alloc = alloc,
		.free = free
	};
}

RefPtr RefPtr_createx(void *ptr, ObjectFreeFunc free) {
	return RefPtr_create(ptr, Platform_instance.alloc, free);
}

Bool RefPtr_inc(RefPtr *ptr) {

	if(!ptr || !ptr->free)
		return false;

	AtomicI64_inc(&ptr->refCount);
	return true;
}

Bool RefPtr_dec(RefPtr **pptr) {

	if(!pptr || !*pptr)
		return false;

	RefPtr *ptr = *pptr;

	Bool b = true;

	if(!AtomicI64_dec(&ptr->refCount)) {
		b = ptr->free(ptr->ptr, ptr->alloc);
		*ptr = (RefPtr) { 0 };
	}

	*pptr = NULL;
	return b;
}
