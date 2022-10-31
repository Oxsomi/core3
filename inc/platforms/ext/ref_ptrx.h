#pragma once
#include "platforms/platform.h"
#include "types/ref_ptr.h"

inline RefPtr RefPtr_createx(void *ptr, ObjectFreeFunc free) {
	return RefPtr_create(ptr, Platform_instance.alloc, free)
}
