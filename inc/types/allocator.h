/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef Error (*AllocFunc)(void *allocator, U64 length, Buffer *output);

//Free should only return if it successfully freed.
//It shouldn't return any errors, as freeing also happens on cleanup.
//This could bring the program into an invalid state.
//
typedef Bool (*FreeFunc)(void *allocator, Buffer buf);

typedef struct Allocator {
	void *ptr;
	AllocFunc alloc;
	FreeFunc free;
} Allocator;

#ifdef __cplusplus
	}
#endif
