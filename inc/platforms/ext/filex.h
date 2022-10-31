#pragma once
#include "file/file.h"
#include "platform/platform.h"

inline Error File_readx(String loc, Buffer *output) {
	return File_read(loc, Platform_instance.alloc, output);
}
