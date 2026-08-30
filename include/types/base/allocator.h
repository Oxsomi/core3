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

//types/base/allocator.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Buffer Buffer;
typedef struct Error Error;

//Alignment contract: whatever an AllocFunc hands back has to be aligned for OxC3's widest type, I32x4,
// which is alignas(16) on every SIMD backend (types/math/vec4_{sse,neon,none}.inc.h).
//Anything less is undefined behaviour for every heap stored vector/matrix,
// and AES-GCM rejects such a buffer outright (buffer_encrypt.c requires 16-byte aligned target/additionalData).
//
//A plain malloc satisfies this on the x64 and arm64 ABIs, where alignof(max_align_t) is 16.
//It does NOT on wasm, where it is 8:
// allocators used on the web target must go through aligned_alloc(16, ...) (or posix_memalign) instead.
//OxC3's own allocators do this already, see platforms/unix/uplatform.c,
// types/container/test/shared/basic_alloc.c and types/container/perf/exec/perf_main.c.

typedef Bool (*AllocFunc)(void *allocator, U64 length, Buffer *output, Error *e_rr);

//Free should only return if it successfully freed.
//It shouldn't return any errors, as freeing also happens on cleanup.
//This could bring the program into an invalid state.
//
typedef void (*FreeFunc)(void *allocator, Buffer buf);

typedef struct Allocator {
	void *ptr;
	AllocFunc alloc;
	FreeFunc free;
} Allocator;

#ifdef __cplusplus
	}
#endif
