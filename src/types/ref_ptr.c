#include "types/ref_ptr.h"

Bool RefPtr_add(RefPtr *ptr) {

	if(!ptr || !ptr->refCount)
		return false;

	++ptr->refCount;
	return true;
}

Bool RefPtr_sub(RefPtr *ptr) {

	if(!ptr || !ptr->refCount)
		return false;

	Bool b = true;

	if(!(--ptr->refCount)) {
		b = ptr->free(ptr->ptr, ptr->alloc);
		*ptr = (RefPtr) { 0 };
	}

	return b;
}
