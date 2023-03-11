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

#include "platforms/ext/buffer_layoutx.h"
#include "platforms/platform.h"

Error BufferLayout_createx(BufferLayout *layout) {
	return BufferLayout_create(Platform_instance.alloc, layout);
}

Bool BufferLayout_freex(BufferLayout *layout) {
	return BufferLayout_free(Platform_instance.alloc, layout);
}

Error BufferLayout_createStructx(BufferLayout *layout, BufferLayoutStructInfo info, U32 *id) {
	return BufferLayout_createStruct(layout, info, Platform_instance.alloc, id);
}

Error BufferLayout_createInstancex(BufferLayout layout, U64 count, Buffer *result) {
	return BufferLayout_createInstance(layout, count, Platform_instance.alloc, result);
}

Error BufferLayout_resolveLayoutx(BufferLayout layout, CharString path, LayoutPathInfo *info) {
	return BufferLayout_resolveLayout(layout, path, info, Platform_instance.alloc);
}

Error BufferLayout_resolvex(Buffer buffer, BufferLayout layout, CharString path, Buffer *location) {
	return BufferLayout_resolve(buffer, layout, path, location, Platform_instance.alloc);
}

//Setting data in the buffer

Error BufferLayout_setDatax(Buffer buffer, BufferLayout layout, CharString path, Buffer newData) {
	return BufferLayout_setData(buffer, layout, path, newData, Platform_instance.alloc);
}

Error BufferLayout_getDatax(Buffer buffer, BufferLayout layout, CharString path, Buffer *currentData) {
	return BufferLayout_getData(buffer, layout, path, currentData, Platform_instance.alloc);
}

//Auto generated getters and setters

#define _BUFFER_LAYOUT_SGET2_IMPL(T)																\
																									\
Error BufferLayout_set##T##x(Buffer buffer, BufferLayout layout, CharString path, T t) {			\
	return BufferLayout_set##T(buffer, layout, path, t, Platform_instance.alloc);					\
}																									\
																									\
Error BufferLayout_get##T##x(Buffer buffer, BufferLayout layout, CharString path, T *t) {			\
	return BufferLayout_get##T(buffer, layout, path, t, Platform_instance.alloc);					\
}

#define _BUFFER_LAYOUT_XINT_SGET2_IMPL(prefix, suffix)												\
_BUFFER_LAYOUT_SGET2_IMPL(prefix##8##suffix);														\
_BUFFER_LAYOUT_SGET2_IMPL(prefix##16##suffix);														\
_BUFFER_LAYOUT_SGET2_IMPL(prefix##32##suffix);														\
_BUFFER_LAYOUT_SGET2_IMPL(prefix##64##suffix);

#define _BUFFER_LAYOUT_VEC_SGET2_IMPL(prefix)														\
_BUFFER_LAYOUT_SGET2_IMPL(prefix##32##x2);															\
_BUFFER_LAYOUT_SGET2_IMPL(prefix##32##x4)

//Setters for C8, Bool and ints/uints

_BUFFER_LAYOUT_SGET2_IMPL(C8);
_BUFFER_LAYOUT_SGET2_IMPL(Bool);

_BUFFER_LAYOUT_XINT_SGET2_IMPL(U, );
_BUFFER_LAYOUT_XINT_SGET2_IMPL(I, );

_BUFFER_LAYOUT_SGET2_IMPL(F32);
_BUFFER_LAYOUT_SGET2_IMPL(F64);

_BUFFER_LAYOUT_VEC_SGET2_IMPL(I);
_BUFFER_LAYOUT_VEC_SGET2_IMPL(F);

//Foreach, etc.

Error BufferLayout_foreachx(
	BufferLayout layout,
	CharString path,
	BufferLayoutForeachFunc func,
	void *userData,
	Bool isRecursive
) {
	return BufferLayout_foreach(
		layout, path,
		func, userData,
		isRecursive,
		Platform_instance.alloc
	);
}
