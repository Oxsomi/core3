#include "types/ref_ptr.h"

bool RefPtr_add(RefPtr *ptr) {

	if(!ptr || !ptr->refCount)
		return false;

	++ptr->refCount;
	return true;
}

bool RefPtr_sub(RefPtr *ptr) {

	if(!ptr || !ptr->refCount)
		return false;

	if(!(--ptr->refCount)) {
		ptr->free(ptr->ptr, ptr->alloc);
		*ptr = (RefPtr) { 0 };
	}

	return true;
}
