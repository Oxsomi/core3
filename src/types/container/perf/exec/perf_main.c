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

//types/container/perf/exec/perf_main.c

#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/base/atomic.h"
#include "types/base/buffer_base.h"
#include "types/base/string_base.h"
#include "types/container/perf/container_perf.h"

#include <stdlib.h>

//OxC3's widest type is I32x4, alignas(16) on every SIMD backend (see types/math/vec4_*.inc.h),
// so an allocator feeding OxC3 has to return memory that can hold one.
//malloc only promises alignof(max_align_t): that IS 16 on the x64/arm64 ABIs, but only 8 on wasm.
//See the allocator contract in types/base/allocator.h.

#if _PLATFORM_TYPE == PLATFORM_WEB
	//aligned_alloc wants a size that's a multiple of the alignment (C11 7.22.3.1)
	#define OXC3_RAW_ALLOC(len) aligned_alloc(16, (len) ? (((len) + 15) &~ (U64)15) : 16)
#else
	#define OXC3_RAW_ALLOC(len) malloc(len)
#endif

static AtomicI64 allocCounter = { 0 };
static AtomicI64 allocBytes = { 0 };

Bool ourAlloc(void *allocator, U64 length, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;

	(void)allocator;

	if (!output)
		retError(clean, Error_nullPointer(2, "ourAlloc()::output is required"));

	void *ptr = OXC3_RAW_ALLOC(length);

	if (!ptr)
		retError(clean, Error_outOfMemory(0, "ourAlloc() malloc failed"));

	*output = Buffer_createManagedPtr(ptr, length);
	AtomicI64_inc(&allocCounter);
	AtomicI64_add(&allocBytes, (I64)length);

clean:
	return s_uccess;
}

void ourFree(void *allocator, Buffer buf) {
	(void)allocator;
	free((void *)buf.ptrNonConst);
	AtomicI64_dec(&allocCounter);
	AtomicI64_sub(&allocBytes, (I64)Buffer_length(buf));
}

int main() {

	const Allocator alloc = (Allocator){
		.alloc = ourAlloc,
		.free = ourFree,
		.ptr = NULL
	};

	Bool s_uccess = true;
	Error err = Error_none();

	const CharString outputCsv = CharString_createRefCStrConst("test.csv");

	gotoIfError3(clean, Perf_aesThroughput(&alloc, &outputCsv, true, &err));

clean:
	return !s_uccess;
}
