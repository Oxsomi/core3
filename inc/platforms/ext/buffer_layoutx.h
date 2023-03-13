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

#pragma once
#include "types/buffer_layout.h"

Error BufferLayout_createx(BufferLayout *layout);
Bool BufferLayout_freex(BufferLayout *layout);

Error BufferLayout_createStructx(BufferLayout *layout, BufferLayoutStructInfo info, U32 *id);

Error BufferLayout_createInstancex(BufferLayout layout, U64 count, Buffer *result);

Error BufferLayout_resolveLayoutx(BufferLayout layout, CharString path, CharString *parent, LayoutPathInfo *info);
Error BufferLayout_resolvex(Buffer buffer, BufferLayout layout, CharString path, Buffer *location);

//Setting data in the buffer

Error BufferLayout_setDatax(Buffer buffer, BufferLayout layout, CharString path, Buffer newData);
Error BufferLayout_getDatax(Buffer buffer, BufferLayout layout, CharString path, Buffer *currentData);

//Auto generated getters and setters

#define _BUFFER_LAYOUT_SGET2(T)																		\
Error BufferLayout_set##T##x(Buffer buffer, BufferLayout layout, CharString path, T t);				\
Error BufferLayout_get##T##x(Buffer buffer, BufferLayout layout, CharString path, T *t);

#define _BUFFER_LAYOUT_XINT_SGET2(prefix, suffix)													\
_BUFFER_LAYOUT_SGET2(prefix##8##suffix);															\
_BUFFER_LAYOUT_SGET2(prefix##16##suffix);															\
_BUFFER_LAYOUT_SGET2(prefix##32##suffix);															\
_BUFFER_LAYOUT_SGET2(prefix##64##suffix);

#define _BUFFER_LAYOUT_VEC_SGET2(prefix)															\
_BUFFER_LAYOUT_SGET2(prefix##32##x2);																\
_BUFFER_LAYOUT_SGET2(prefix##32##x4)

//Setters for C8, Bool and ints/uints

_BUFFER_LAYOUT_SGET2(C8);
_BUFFER_LAYOUT_SGET2(Bool);

_BUFFER_LAYOUT_XINT_SGET2(U, );
_BUFFER_LAYOUT_XINT_SGET2(I, );

_BUFFER_LAYOUT_SGET2(F32);
_BUFFER_LAYOUT_SGET2(F64);

_BUFFER_LAYOUT_VEC_SGET2(I);
_BUFFER_LAYOUT_VEC_SGET2(F);

//Foreach, etc.

Error BufferLayout_foreachx(
	BufferLayout layout, 
	CharString path, 
	BufferLayoutForeachFunc func, 
	void *userData,
	Bool isRecursive
);
